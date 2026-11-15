/* The SDL 3 backend's presentation (backends.h): the frame at its whole scale or filling the screen, the band the
 * sub-pixel camera moves with Link's layer (through the logical presentation, or where the touch controls' and the
 * screen margin's own drawing puts the frame), and the text box Text size shows again larger. */
#include "sdl_backend_internal.h"
#include "floating.h"
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

/* The status bar's floating panels over all of it, unmoved, from their texture as it was last uploaded (sb_upload_panels),
 * group by group; through the shader filter as Link's layer goes, its texels on the frame's grid. */
static int draw_panels(sdl_backend *backend, const SDL_FRect *to, float sx, float sy)
{
    if (!backend->panels_shown || !backend->panels || backend->panels_half) return 1;   /* at half the game's scale: after the picture */
    SDL_ScaleMode mode = SDL_SCALEMODE_NEAREST;
    SDL_GetTextureScaleMode(backend->texture, &mode);
    SDL_SetTextureScaleMode(backend->panels, mode);
    const int apart = oracles_sdl_filter_layer(backend->filter) != ORACLES_SDL_LAYER_ALONE;
    /* Straight texels drawn without the shader that premultiplies them (the picture too small for it): blended straight. */
    SDL_SetTextureBlendMode(backend->panels, backend->panels_straight && !apart ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_BLEND_PREMULTIPLIED);
    if (apart) oracles_sdl_filter_part(backend->filter, 1, 0.0f, 0.0f);
    int drawn = 1;
    for (unsigned g = 0; g < backend->panels_groups && drawn; g++) {
        const int *d = backend->panels_draw[g];
        const SDL_FRect src = { (float)d[0], (float)d[1], (float)d[2], (float)d[3] };
        drawn = sb_put(backend->renderer, backend->panels, &src, &src, to, sx, sy);
    }
    if (apart) oracles_sdl_filter_part(backend->filter, -1, 0.0f, 0.0f);
    return drawn;
}

/* The texels a sampler or the filter's shader reads around a part it draws: Smooth's half texel and the shaders' reach,
 * and CRT's curve twice besides where it bends (the part drawn wider by it, and its reads moved by it). */
#define PANELS_RING (1 + LAYER_REACH)

/* A group's part of the panels (x, y, w, h of `panels`, `pitch` texels a line, as far outside it as a ring takes it), only
 * the texels inside `keep` (x0, y0, x1, y1) taken, the others transparent: premultiplied as they are, or (`straight`) as a
 * shader that keeps the colour to the coverage of the texel it reads takes it (ORACLES_SDL_LAYER_OVER_GROUND): each colour
 * straight, and the transparent texels as far as the shader reads around an opaque one carrying the colour of the nearest,
 * so that the panels' edges are filtered against themselves rather than against black; into backend->panels_pixels, `w`
 * texels a line.  0 without the memory. */
static int part_texels(sdl_backend *backend, const uint32_t *panels, int pitch, const int at[4], const int keep[4], int straight)
{
    const size_t count = (size_t)at[2] * (size_t)at[3];
    if (backend->panels_pixels_size < count) {
        uint32_t *pixels = SDL_realloc(backend->panels_pixels, count * sizeof *pixels);
        if (!pixels) return 0;
        backend->panels_pixels = pixels;
        uint8_t *reach = SDL_realloc(backend->panels_reach, count);
        if (!reach) return 0;
        backend->panels_reach = reach;
        backend->panels_pixels_size = count;
    }
    uint32_t *out = backend->panels_pixels;
    uint8_t *reach = backend->panels_reach;
    for (int y = 0; y < at[3]; y++)
        for (int x = 0; x < at[2]; x++) {
            const int sx = at[0] + x, sy = at[1] + y;
            const uint32_t p = sx >= keep[0] && sy >= keep[1] && sx < keep[2] && sy < keep[3] ? panels[(size_t)sy * (size_t)pitch + (size_t)sx] : 0u, a = p >> 24;
            uint32_t texel = straight ? 0u : p;
            if (a && straight) {
                texel = a << 24;
                for (unsigned shift = 0; shift < 24u; shift += 8u) texel |= SDL_min((((p >> shift) & 0xffu) * 255u + a / 2u) / a, 255u) << shift;
            }
            out[(size_t)y * (size_t)at[2] + (size_t)x] = texel;
            reach[(size_t)y * (size_t)at[2] + (size_t)x] = a ? 0u : 0xffu;
        }
    for (unsigned step = 1; straight && step <= LAYER_REACH + 1; step++)
        for (int y = 0; y < at[3]; y++)
            for (int x = 0; x < at[2]; x++) {
                const size_t i = (size_t)y * (size_t)at[2] + (size_t)x;
                if (reach[i] != 0xffu) continue;
                static const int around[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };
                for (unsigned k = 0; k < 4u; k++) {
                    const int nx = x + around[k][0], ny = y + around[k][1];
                    if (nx < 0 || ny < 0 || nx >= at[2] || ny >= at[3]) continue;
                    const size_t j = (size_t)ny * (size_t)at[2] + (size_t)nx;
                    if (reach[j] != step - 1u) continue;
                    out[i] = out[j] & 0x00ffffffu;
                    reach[i] = (uint8_t)step;
                    break;
                }
            }
    return 1;
}

/* The panels' texture made w x h (`exact`), or at least that large, the one there kept when it is. */
static int make_panels(sdl_backend *backend, int w, int h, int exact)
{
    if (backend->panels && (exact ? backend->panels_w == w && backend->panels_h == h : backend->panels_w >= w && backend->panels_h >= h)) return 1;
    if (!exact) { w = SDL_max(w, backend->panels_w); h = SDL_max(h, backend->panels_h); }
    if (backend->panels) SDL_DestroyTexture(backend->panels);
    backend->panels = SDL_CreateTexture(backend->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
    backend->panels_w = backend->panels ? w : 0; backend->panels_h = backend->panels ? h : 0;
    if (!backend->panels) return 0;
    SDL_SetTextureBlendMode(backend->panels, SDL_BLENDMODE_BLEND_PREMULTIPLIED);
    return 1;
}

/* The panels at half the game's scale, in their layer w x h: each kept to a corner of its own, so each alone in the
 * texture, side by side, with as much around it as the filter reads where it draws it (Smooth's half texel, the
 * shaders' reach and CRT's curve), transparent but for its own texels: none of a neighbour's read, which the screen puts
 * elsewhere.  None around them without a filter: drawn at a whole number of screen pixels a pixel, nothing read around
 * them. */
static int upload_half_panels(sdl_backend *backend, const oracles_host_video_subpixel *sp, int w, int h)
{
    /* A shader filter's ring whether the picture is large enough for it or not, as the pause may draw it larger. */
    const int bend = oracles_sdl_filter_bend(backend->filter, w, h);
    const int ring = backend->filter ? PANELS_RING + 2 * bend : oracles_sdl_filter_smooth() ? 1 : 0;
    backend->panels_straight = oracles_sdl_filter_chosen_layer(backend->filter) == ORACLES_SDL_LAYER_OVER_GROUND;   /* blended straight where no shader draws them */
    backend->panels_bend = bend;
    int parts[ORACLES_HOST_SUBPIXEL_PANELS][4], keep[ORACLES_HOST_SUBPIXEL_PANELS][4], at[ORACLES_HOST_SUBPIXEL_PANELS][2];   /* in the layer; in the texture */
    unsigned n = 0;
    int across = 0, down = 0;
    for (uint32_t i = 0; i < sp->panels_count && i < ORACLES_HOST_SUBPIXEL_PANELS; i++) {
        const uint32_t *r = sp->panels_rects[i];
        if (!r[2] || !r[3] || r[0] >= (uint32_t)w || r[1] >= (uint32_t)h || r[2] > (uint32_t)w - r[0] || r[3] > (uint32_t)h - r[1]) continue;
        const int x = (int)r[0] - ring, y = (int)r[1] - ring;
        parts[n][0] = x; parts[n][1] = y; parts[n][2] = (int)r[2] + 2 * ring; parts[n][3] = (int)r[3] + 2 * ring;
        keep[n][0] = (int)r[0]; keep[n][1] = (int)r[1]; keep[n][2] = (int)(r[0] + r[2]); keep[n][3] = (int)(r[1] + r[3]);
        at[n][0] = across; at[n][1] = 0;
        across = at[n][0] + parts[n][2];
        down = SDL_max(down, at[n][1] + parts[n][3]);
        int *d = backend->panels_draw[n];
        d[0] = (int)r[0]; d[1] = (int)r[1]; d[2] = (int)r[2]; d[3] = (int)r[3];
        backend->panels_at[n][0] = at[n][0] + ring; backend->panels_at[n][1] = at[n][1] + ring;
        backend->panels_anchor[n] = sp->panels_anchor[i];
        n++;
    }
    if (!n || !make_panels(backend, across, down, 0)) return 0;
    int uploaded = 1;
    for (unsigned g = 0; g < n && uploaded; g++) {
        const SDL_Rect to = { at[g][0], at[g][1], parts[g][2], parts[g][3] };
        uploaded = part_texels(backend, sp->panels, w, parts[g], keep[g], backend->panels_straight)
                   && SDL_UpdateTexture(backend->panels, &to, backend->panels_pixels, to.w * (int)sizeof *backend->panels_pixels);
    }
    backend->panels_groups = n;
    return backend->panels_shown = uploaded;
}

/* The panels' texture, the frame's size, written where they stand and around them, transparent, as far as what draws them
 * reads: the rest of the texture is never read.  Panels whose rings meet are one group, uploaded and drawn as one, so
 * that no texel is drawn twice; drawn as wide as CRT's curve moves them.  Premultiplied, or straight for a shader that
 * keeps the colour to the coverage of the texel it reads. */
int sb_upload_panels(sdl_backend *backend, const oracles_host_video_subpixel *sp)
{
    backend->panels_shown = 0;
    backend->panels_groups = 0;
    backend->panels_half = sp->panels_scale == 2u;
    if (!sp->panels || !sp->panels_count) return 0;
    const int half = backend->panels_half ? 2 : 1, w = half * backend->frame_width, h = half * backend->frame_height;
    if (half > 1) return upload_half_panels(backend, sp, w, h);
    if (!make_panels(backend, w, h, 1)) return 0;
    const int bend = oracles_sdl_filter_bend(backend->filter, w, h), ring = PANELS_RING + 2 * bend, whole[4] = { 0, 0, w, h };
    backend->panels_straight = oracles_sdl_filter_chosen_layer(backend->filter) == ORACLES_SDL_LAYER_OVER_GROUND;   /* blended straight where no shader draws them */
    int up[ORACLES_HOST_SUBPIXEL_PANELS][4], draw[ORACLES_HOST_SUBPIXEL_PANELS][4];   /* x0, y0, x1, y1 */
    unsigned n = 0;
    for (uint32_t i = 0; i < sp->panels_count && i < ORACLES_HOST_SUBPIXEL_PANELS; i++) {
        const uint32_t *r = sp->panels_rects[i];
        if (!r[2] || !r[3] || r[0] >= (uint32_t)w || r[1] >= (uint32_t)h || r[2] > (uint32_t)w - r[0] || r[3] > (uint32_t)h - r[1]) continue;
        draw[n][0] = (int)r[0]; draw[n][1] = (int)r[1]; draw[n][2] = (int)(r[0] + r[2]); draw[n][3] = (int)(r[1] + r[3]);
        up[n][0] = SDL_max(draw[n][0] - ring, 0); up[n][1] = SDL_max(draw[n][1] - ring, 0);
        up[n][2] = SDL_min(draw[n][2] + ring, w); up[n][3] = SDL_min(draw[n][3] + ring, h);
        n++;
    }
    for (int merged = 1; merged;) {   /* groups whose uploaded parts meet, as one */
        merged = 0;
        for (unsigned i = 0; i < n && !merged; i++)
            for (unsigned j = i + 1; j < n && !merged; j++) {
                if (up[i][0] >= up[j][2] || up[j][0] >= up[i][2] || up[i][1] >= up[j][3] || up[j][1] >= up[i][3]) continue;
                for (unsigned k = 0; k < 2u; k++) {
                    up[i][k] = SDL_min(up[i][k], up[j][k]); up[i][k + 2] = SDL_max(up[i][k + 2], up[j][k + 2]);
                    draw[i][k] = SDL_min(draw[i][k], draw[j][k]); draw[i][k + 2] = SDL_max(draw[i][k + 2], draw[j][k + 2]);
                }
                memmove(up[j], up[j + 1], (n - j - 1) * sizeof up[0]);
                memmove(draw[j], draw[j + 1], (n - j - 1) * sizeof draw[0]);
                n--;
                merged = 1;
            }
    }
    int uploaded = n > 0;
    for (unsigned g = 0; g < n; g++) {
        const SDL_Rect at = { up[g][0], up[g][1], up[g][2] - up[g][0], up[g][3] - up[g][1] };
        const int part[4] = { at.x, at.y, at.w, at.h };
        if (!backend->panels_straight)
            uploaded = uploaded && SDL_UpdateTexture(backend->panels, &at, sp->panels + (size_t)at.y * (size_t)w + (size_t)at.x, w * (int)sizeof *sp->panels);
        else uploaded = uploaded && part_texels(backend, sp->panels, w, part, whole, 1)
                        && SDL_UpdateTexture(backend->panels, &at, backend->panels_pixels, at.w * (int)sizeof *backend->panels_pixels);
        const int x0 = SDL_max(draw[g][0] - bend, 0), y0 = SDL_max(draw[g][1] - bend, 0), x1 = SDL_min(draw[g][2] + bend, w), y1 = SDL_min(draw[g][3] + bend, h);
        backend->panels_draw[g][0] = x0; backend->panels_draw[g][1] = y0; backend->panels_draw[g][2] = x1 - x0; backend->panels_draw[g][3] = y1 - y0;
    }
    backend->panels_groups = n;
    return backend->panels_shown = uploaded;
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
    /* The panels at half the game's scale alone, no move: the frame as it is (they follow the picture). */
    const int still = sp->panels_scale == 2u && !sp->x && !sp->y && !layered;
    if (scale < 2.0f || still) return SDL_RenderTexture(r, backend->texture, NULL, to) && draw_panels(backend, to, to ? to->w / w : 1.0f, to ? to->h / h : 1.0f);
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
    return draw_panels(backend, to, sx, sy);
}

int sb_draw_half_panels(sdl_backend *backend, const SDL_FRect *to, int direct)
{
    if (!backend->shown_subpixel || backend->shown_cropped || !backend->panels_half || !backend->panels_shown || !backend->panels || to->w <= 0.0f || to->h <= 0.0f)
        return 1;
    int logical_w = 0, logical_h = 0;
    SDL_RendererLogicalPresentation mode = SDL_LOGICAL_PRESENTATION_DISABLED;
    if (!direct) {
        SDL_GetRenderLogicalPresentation(backend->renderer, &logical_w, &logical_h, &mode);
        SDL_SetRenderLogicalPresentation(backend->renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);   /* the screen's pixels */
    }
    /* Through the filter as Link's layer goes, as far around each as CRT's curve takes it, Scanlines', CRT's and LCD's
     * masks on the picture's own pixels, the others on the panels' texels; without one, straight texels blended
     * straight. */
    const int apart = oracles_sdl_filter_layer(backend->filter) != ORACLES_SDL_LAYER_ALONE;
    SDL_SetTextureScaleMode(backend->panels, oracles_sdl_filter_smooth() ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(backend->panels, backend->panels_straight && !apart ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_BLEND_PREMULTIPLIED);
    if (apart) oracles_sdl_filter_part_cells(backend->filter, 1);   /* the screen's masks run on from the picture into them */
    const float x0 = SDL_floorf(to->x), y0 = SDL_floorf(to->y);   /* the picture's whole place, as the band's */
    const uint32_t layer_w = 2u * (uint32_t)backend->frame_width, layer_h = 2u * (uint32_t)backend->frame_height;
    const float bend = apart ? (float)backend->panels_bend : 0.0f;
    int drawn = 1;
    for (unsigned g = 0; g < backend->panels_groups && drawn; g++) {
        const int *d = backend->panels_draw[g];
        const int32_t rect[4] = { d[0], d[1], d[2], d[3] };
        float place[4];
        oracles_floating_screen_place(rect, backend->panels_anchor[g], layer_w, layer_h, to->w, to->h, place);
        const float unit = place[2] / (float)d[2];
        const SDL_FRect src = { (float)backend->panels_at[g][0] - bend, (float)backend->panels_at[g][1] - bend, (float)d[2] + 2.0f * bend, (float)d[3] + 2.0f * bend };
        const SDL_FRect dst = { x0 + place[0] - bend * unit, y0 + place[1] - bend * unit, place[2] + 2.0f * bend * unit, place[3] + 2.0f * bend * unit };
        drawn = SDL_RenderTexture(backend->renderer, backend->panels, &src, &dst);
    }
    if (apart) {
        oracles_sdl_filter_part(backend->filter, -1, 0.0f, 0.0f);
        oracles_sdl_filter_use(backend->filter, 0);
    }
    if (!direct) SDL_SetRenderLogicalPresentation(backend->renderer, logical_w, logical_h, mode);
    return drawn;
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
void sb_take_subpixel(sdl_backend *backend, const oracles_host_video_frame *frame)
{
    const oracles_host_video_subpixel *sp = frame->subpixel;
    const int layered = sb_upload_layer(backend, sp->layer, sp->patch, sp, frame->pixels, frame->pitch_bytes);
    backend->last_layer = layered ? sp->layer : NULL;
    backend->last_patch = sp->patch;
    backend->last_panels = sp->panels;
    sb_upload_panels(backend, sp);
    backend->shown = *sp;
    backend->shown.layer = backend->shown.patch = NULL;   /* the view's, which change: the layer's texture keeps them */
    backend->shown.panels = NULL;                         /* and the panels' texture theirs */
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
    sb_take_subpixel(backend, frame);
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
    oracles_sdl_filter_use(backend->filter, 0);   /* the box, the touch controls and the margin unfiltered; the panels at half the game's scale through it again */
    sb_draw_half_panels(backend, to, 1);
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
            sb_take_subpixel(backend, frame);
            drawn = sb_draw_subpixel(backend, &backend->shown, backend->shown_layered, NULL);
        }
        else drawn = SDL_RenderTexture(backend->renderer, backend->texture, cropped ? &part : NULL, NULL);
        oracles_sdl_filter_use(backend->filter, 0);
        if (!drawn) return 0;
        SDL_FRect to;   /* the panels at half the game's scale and the box above the band, Link and the fixed rectangles, at the frame's whole place */
        if (SDL_GetRenderLogicalPresentationRect(backend->renderer, &to)) {
            sb_draw_half_panels(backend, &to, 0);
            draw_box(backend, &to, 0);
        }
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
