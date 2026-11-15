/* The SDL 3 backend's presentation (backends.h): the frame at its whole scale or filling the screen, the band the
 * sub-pixel camera moves with Link's layer (through the logical presentation, or where the touch controls' and the
 * screen margin's own drawing puts the frame), and the text box Text size shows again larger. */
#include "sdl_backend_internal.h"
#include "sdl_filter.h"

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

/* The layer's texture: three blocks of (pitch + 2 margin) texels square side by side, each holding its part from
 * (margin, margin): Link premultiplied, transparent around him (drawn alone, sampled smoothly or not); Link over the
 * ground under him and the ground itself, the frame's pixels around them as far as the margin (drawn through a shader,
 * which reads LAYER_REACH texels away from where it stands, and as many more as its curve moves it, the ground drawn as
 * much wider; or the ground alone, a pixel wider, under him drawn alone). */
#define LAYER_REACH 2

/* The layer's part from (u, v), w x h, drawn at (x, y) of the frame, cut to the band (none over the status bar or below
 * the band). */
static int put_in_band(SDL_Renderer *r, SDL_Texture *layer, float u, float v, float x, float y, float w, float h, float top, float bh, const SDL_FRect *to,
                       float sx, float sy)
{
    const float cut = top > y ? top - y : 0.0f;
    if (y + h > top + bh) h = top + bh - y;
    h -= cut;
    if (h <= 0.0f) return 1;
    const SDL_FRect src = { u, v + cut, w, h }, dst = { x, y + cut, w, h };
    return sb_put(r, layer, &src, &dst, to, sx, sy);
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
        const int apart = oracles_sdl_filter_layer(backend->filter);
        SDL_ScaleMode mode = SDL_SCALEMODE_NEAREST;
        SDL_GetTextureScaleMode(backend->texture, &mode);
        SDL_SetTextureScaleMode(backend->layer, mode);
        const float margin = (float)backend->layer_margin, bend = (float)backend->layer_bend;
        const float side = (float)sp->layer_pitch + 2.0f * margin, lw = (float)sp->layer_w, lh = (float)sp->layer_h;
        const float lx = sp->layer_fx ? screen_fraction_down(sp->layer_fx + sp->x, scale) - fx : 0.0f;
        const float ly = sp->layer_fy ? screen_fraction_down(sp->layer_fy + sp->y, scale) - fy : 0.0f;
        const float x = (float)sp->layer_x, y = (float)sp->layer_y;
        if (apart == ORACLES_SDL_LAYER_ALONE) {
            /* The ground where the band puts the place he stands, a pixel of the band around it, opaque: a patch
             * transparent around him, sampled smoothly, would blend into its edge the band's drawing of him at his
             * whole pixels, which his own fraction uncovers.  Then him at his own place. */
            const float gx0 = SDL_max(x - 1.0f, 0.0f), gx1 = SDL_min(x + lw + 1.0f, w);
            const float gy0 = SDL_max(y - 1.0f, top), gy1 = SDL_min(y + lh + 1.0f, top + bh);
            if (gx1 > gx0 && gy1 > gy0
                && !put_in_band(r, backend->layer, 2.0f * side + margin + gx0 - x, margin + gy0 - y, gx0 - fx, gy0 - fy, gx1 - gx0, gy1 - gy0, top, bh, to, sx, sy))
                return 0;
        } else {
            /* Through a shader: the ground under him, where the band puts it, and as far around it as the shader read
             * him in the band, the frame's pixels about it read as the band reads them; then him over the ground he
             * stands on, kept to his own pixels by the shader, as far around him as its curve may take them.  Each
             * part's grid against the band's: the band's texel under it is the frame's place plus the band's fraction. */
            const float ring = LAYER_REACH + bend;
            const float gx0 = SDL_max(x - ring, 0.0f), gx1 = SDL_min(x + lw + ring, w);
            const float gy0 = SDL_max(y - ring, top), gy1 = SDL_min(y + lh + ring, top + bh);
            const float u = 2.0f * side + margin + gx0 - x, v = margin + gy0 - y;
            oracles_sdl_filter_part(backend->filter, 0, gx0 - u, gy0 - v);
            if (gx1 > gx0 && gy1 > gy0 && !put_in_band(r, backend->layer, u, v, gx0 - fx, gy0 - fy, gx1 - gx0, gy1 - gy0, top, bh, to, sx, sy)) {
                oracles_sdl_filter_part(backend->filter, -1, 0.0f, 0.0f);
                return 0;
            }
        }
        const float u = (apart == ORACLES_SDL_LAYER_OVER_GROUND ? side : 0.0f) + margin - bend, v = margin - bend;
        if (apart != ORACLES_SDL_LAYER_ALONE) oracles_sdl_filter_part(backend->filter, 1, x - bend + lx + fx - u, y - bend + ly + fy - v);
        const int drawn = put_in_band(r, backend->layer, u, v, x - bend + lx, y - bend + ly, lw + 2.0f * bend, lh + 2.0f * bend, top, bh, to, sx, sy);
        if (apart != ORACLES_SDL_LAYER_ALONE) oracles_sdl_filter_part(backend->filter, -1, 0.0f, 0.0f);
        if (!drawn) return 0;
    }
    for (uint32_t i = 0; i < sp->fixed_count && i < ORACLES_HOST_SUBPIXEL_FIXED; i++) {
        src = (SDL_FRect){ (float)sp->fixed[i][0], (float)sp->fixed[i][1], (float)sp->fixed[i][2], (float)sp->fixed[i][3] };
        if (!sb_put(r, backend->texture, &src, &src, to, sx, sy)) return 0;
    }
    return 1;
}

static uint32_t premultiplied(uint32_t p)
{
    const uint32_t a = p >> 24, r = ((p >> 16) & 0xffu) * a / 255u, g = ((p >> 8) & 0xffu) * a / 255u, b = (p & 0xffu) * a / 255u;
    return a << 24 | r << 16 | g << 8 | b;
}

/* `p` over the opaque `under`, its colour straight, its alpha kept. */
static uint32_t over(uint32_t p, uint32_t under)
{
    const uint32_t a = p >> 24;
    uint32_t out = a << 24;
    for (unsigned shift = 0; shift < 24; shift += 8)
        out |= ((((p >> shift) & 0xffu) * a + ((under >> shift) & 0xffu) * (255u - a)) / 255u) << shift;
    return out;
}

/* Link's layer and the patch under him, w x h pixels `pitch` apart, into the layer's three blocks, with a margin of
 * 2 (LAYER_REACH + the filter's curve) texels, as far as the filter chosen reads: the texture written whole when it is
 * made, then the blocks' parts and margins, all that a sampler or a shader reads, at each frame, in one rectangle;
 * nothing it reads is left as the GPU's memory had it.  Link premultiplied goes with the texture's premultiplied
 * blending: sampled smoothly (Smooth, Fill), his edge blends half a texel of transparent and shows the ground beneath
 * it.  The ground is the frame's pixels with the patch over them (the frame's edge pixels where the margin passes it,
 * patched as they are), opaque; Link over it carries the ground's colour where he is transparent, for a shader to
 * read, and his own alpha, which the shader keeps him to. */
int sb_upload_layer(sdl_backend *backend, const uint32_t *layer, const uint32_t *patch, const oracles_host_video_subpixel *at, const uint32_t *frame,
                    size_t frame_pitch)
{
    const uint32_t w = at->layer_w, h = at->layer_h, pitch = at->layer_pitch;
    if (!layer || !patch || !w || !h || w > pitch || h > pitch || pitch > 1024) return 0;
    const int fw = backend->frame_width, fh = backend->frame_height;
    const int bend = oracles_sdl_filter_bend(backend->filter, fw, fh), margin = 2 * LAYER_REACH + 2 * bend;
    const int side = (int)pitch + 2 * margin;
    if (!backend->layer || backend->layer_w != 3 * side || backend->layer_h != side) {
        if (backend->layer) SDL_DestroyTexture(backend->layer);
        backend->layer_w = 3 * side; backend->layer_h = side;
        backend->layer = SDL_CreateTexture(backend->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, backend->layer_w, backend->layer_h);
        backend->layer_written = 0;
        if (!backend->layer) { backend->layer_w = backend->layer_h = 0; return 0; }   /* him with the band for a frame rather than no frame */
        SDL_SetTextureBlendMode(backend->layer, SDL_BLENDMODE_BLEND_PREMULTIPLIED);
    }
    backend->layer_margin = margin;
    backend->layer_bend = bend;
    const size_t count = (size_t)backend->layer_w * (size_t)backend->layer_h;
    if (backend->layer_pixels_size < count) {
        uint32_t *pixels = SDL_realloc(backend->layer_pixels, count * sizeof *pixels);
        if (!pixels) return 0;
        backend->layer_pixels = pixels;
        backend->layer_pixels_size = count;
    }
    uint32_t *out = backend->layer_pixels;
    if (!backend->layer_written || backend->layer_part_w != w || backend->layer_part_h != h) {
        SDL_memset(out, 0, count * sizeof *out);   /* else only the parts change, written whole below, the rest 0 still */
        backend->layer_part_w = w; backend->layer_part_h = h;
    }
    for (int y = -margin; y < (int)h + margin; y++) {
        uint32_t *line = out + (size_t)(y + margin) * (size_t)backend->layer_w + margin;
        const int64_t fy = SDL_clamp((int64_t)at->layer_y + y, 0, (int64_t)fh - 1), py = fy - at->layer_y;
        for (int x = -margin; x < (int)w + margin; x++) {
            const int64_t fx = SDL_clamp((int64_t)at->layer_x + x, 0, (int64_t)fw - 1), px = fx - at->layer_x;
            uint32_t ground = frame ? ((const uint32_t *)((const uint8_t *)frame + (size_t)fy * frame_pitch))[fx] | 0xff000000u : 0xff000000u;
            /* The patch over the frame's pixel the texel takes: its own, or past the frame its edge's. */
            if (px >= 0 && py >= 0 && px < (int64_t)w && py < (int64_t)h) ground = over(patch[(size_t)py * pitch + (size_t)px], ground) | 0xff000000u;
            uint32_t him = 0;
            if (x >= 0 && y >= 0 && x < (int)w && y < (int)h) {
                him = layer[(size_t)y * pitch + (size_t)x];
                line[x] = premultiplied(him);
            }
            line[side + x] = over(him, ground);
            line[2 * side + x] = ground;
        }
    }
    const int pitch_bytes = backend->layer_w * (int)sizeof *out;
    if (!backend->layer_written) return backend->layer_written = SDL_UpdateTexture(backend->layer, NULL, out, pitch_bytes);
    const SDL_Rect parts = { 0, 0, 2 * side + (int)w + 2 * margin, (int)h + 2 * margin };
    return SDL_UpdateTexture(backend->layer, &parts, out, pitch_bytes);
}

/* The layer and the patch uploaded, the frame's move kept for the pause, which shows it as it was. */
static void take_subpixel(sdl_backend *backend, const oracles_host_video_frame *frame)
{
    const oracles_host_video_subpixel *sp = frame->subpixel;
    const int layered = sb_upload_layer(backend, sp->layer, sp->patch, sp, frame->pixels, frame->pitch_bytes);
    backend->last_layer = layered ? sp->layer : NULL;
    backend->last_patch = sp->patch;
    backend->shown = *sp;
    backend->shown.layer = backend->shown.patch = NULL;   /* the view's, which change: the layer's texture keeps them */
    backend->shown_layered = layered;
    backend->shown_subpixel = 1;
}

int sb_draw_subpixel_cut(sdl_backend *backend, const SDL_FRect *to)
{
    /* At its whole pixels and cut to it, as the logical presentation's viewport places and cuts the frame. */
    const SDL_FRect at = { SDL_floorf(to->x), SDL_floorf(to->y), to->w, to->h };
    SDL_Rect cut = { (int)at.x, (int)at.y, (int)at.w, (int)at.h };
    const bool clipped = SDL_RenderClipEnabled(backend->renderer);
    SDL_Rect before = { 0, 0, 0, 0 };
    if (clipped) SDL_GetRenderClipRect(backend->renderer, &before);
    if (clipped && !SDL_GetRectIntersection(&before, &cut, &cut)) return 1;   /* inside a clip already set: the preview's */
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
    oracles_sdl_filter_place(backend->filter, to, part ? part->w : (float)backend->frame_width, part ? part->h : (float)backend->frame_height);
    oracles_sdl_filter_use(backend->filter, 1);   /* the shader set again for this picture's size */
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
    sdl_backend *backend = opaque;
    oracles_sdl_filter_use(backend->filter, 0);   /* the box, the touch controls and the margin unfiltered */
    draw_box(backend, to, 1);
}

int sb_sdl_present_frame(void *opaque, const oracles_host_video_frame *frame)
{
    sdl_backend *backend = opaque;
    if (frame->pitch_bytes > INT_MAX) return 0;
    const uint64_t upload_started_ns = sb_sdl_monotonic_ns(backend);
    if (!SDL_UpdateTexture(backend->texture, NULL, frame->pixels, (int)frame->pitch_bytes)) return 0;
    backend->last_pixels = frame->pixels;
    backend->last_pitch = frame->pitch_bytes;
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
    oracles_sdl_filter_check(backend->filter, 0);
    oracles_sdl_filter_use(backend->filter, 1);   /* the game's picture through the filter, nothing over it */
    const int direct = oracles_touch_sdl_frame(&backend->touch, backend->renderer, backend->texture, cropped ? &part : NULL);   /* the touch controls' own drawing */
    oracles_sdl_filter_use(backend->filter, 0);
    if (!direct) {
        if (!SDL_RenderClear(backend->renderer)) return 0;
        SDL_FRect picture;
        if (SDL_GetRenderLogicalPresentationRect(backend->renderer, &picture)) oracles_sdl_filter_place(backend->filter, &picture, (float)logical_w, (float)logical_h);
        oracles_sdl_filter_use(backend->filter, 1);
        int drawn;
        if (frame->subpixel && !cropped) {
            take_subpixel(backend, frame);
            drawn = sb_draw_subpixel(backend, &backend->shown, backend->shown_layered, NULL);
        }
        else drawn = SDL_RenderTexture(backend->renderer, backend->texture, cropped ? &part : NULL, NULL);
        oracles_sdl_filter_use(backend->filter, 0);
        if (!drawn) return 0;
        SDL_FRect to;   /* the box above the band, Link and the fixed rectangles, at the frame's whole place */
        if (SDL_GetRenderLogicalPresentationRect(backend->renderer, &to)) draw_box(backend, &to, 0);
    }
    backend->presenting = NULL;
    if (backend->overlay && !backend->overlay(backend->overlay_opaque, backend->renderer)) backend->overlay = NULL;
    const uint64_t present_started_ns = sb_sdl_monotonic_ns(backend);
    SDL_RenderPresent(backend->renderer);
    oracles_sdl_filter_check(backend->filter, 1);
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
