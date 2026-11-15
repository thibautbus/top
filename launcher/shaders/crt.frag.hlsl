// SameBoy's CRT filter (Shaders/CRT.fsh, SameBoy 1.0.3) for SDL's GPU renderer (sameboy.hlsli).  Its scanlines,
// shadow mask and sub-pixels are kept as SameBoy writes them, in the drawn texture's positions moved by `grid`.  Its
// curve is drawn in the picture's place in the window instead (the constants' `image`), so that every part the
// sub-pixel camera draws bends as one picture, and without SameBoy's 8:7 squeeze of the Game Boy's pixels: the picture
// keeps its own proportions.
//
// Copyright (c) 2015-2026 Lior Halphon
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.


#include "sameboy.hlsli"

#define COLOR_LOW 0.45
#define COLOR_HIGH 1.0
#define VERTICAL_BORDER_DEPTH 0.6
#define SCANLINE_DEPTH 0.55
#define CURVENESS 0.3

STATIC vec4 scale(sampler2D image, vec2 position, vec2 input_resolution, vec2 output_resolution)
{
    /* The curve: drawn by main, in the picture's place (below) */

    /* Setting up common vars */
    vec2 pos = fract(position * input_resolution + grid);
    vec2 sub_pos = pos * 6.0;

    vec4 center = texture_relative(image, position, vec2(0, 0));
    vec4 left = texture_relative(image, position, vec2(-1, 0));
    vec4 right = texture_relative(image, position, vec2(1, 0));
    
    /* Vertical blurring */
    if (sub_pos.y < 1.0) {
        center = mix(center, texture_relative(image, position, vec2( 0, -1)), 0.5 - sub_pos.y / 2.0);
        left =   mix(left,   texture_relative(image, position, vec2(-1, -1)), 0.5 - sub_pos.y / 2.0);
        right =  mix(right,  texture_relative(image, position, vec2( 1, -1)), 0.5 - sub_pos.y / 2.0);
    }
    else if (sub_pos.y > 5.0) {
        center = mix(center, texture_relative(image, position, vec2( 0, 1)), (sub_pos.y - 5.0) / 2.0);
        left =   mix(left,   texture_relative(image, position, vec2(-1, 1)), (sub_pos.y - 5.0) / 2.0);
        right =  mix(right,  texture_relative(image, position, vec2( 1, 1)), (sub_pos.y - 5.0) / 2.0);
    }
    
    /* Scanlines */
    float scanline_multiplier;
    if (pos.y < 0.5) {
        scanline_multiplier = (pos.y * 2.0) * SCANLINE_DEPTH + (1.0 - SCANLINE_DEPTH);
    }
    else  {
        scanline_multiplier = ((1.0 - pos.y) * 2.0) * SCANLINE_DEPTH + (1.0 - SCANLINE_DEPTH);
    }
    
    center *= scanline_multiplier;
    left *= scanline_multiplier;
    right *= scanline_multiplier;

    /* Vertical seperator for shadow masks */
    bool odd = bool(int(floor(position * input_resolution + grid).x) & 1);
    if (odd) {
        pos.y += 0.5;
        pos.y = fract(pos.y);
    }
    
    if (pos.y < 1.0 / 3.0) {
        float gradient_position = pos.y * 3.0;
        center *= gradient_position * VERTICAL_BORDER_DEPTH + (1.0 - VERTICAL_BORDER_DEPTH);
        left *= gradient_position * VERTICAL_BORDER_DEPTH + (1.0 - VERTICAL_BORDER_DEPTH);
        right *= gradient_position * VERTICAL_BORDER_DEPTH + (1.0 - VERTICAL_BORDER_DEPTH);
    }
    else if (pos.y > 2.0 / 3.0) {
        float gradient_position = (1.0 - pos.y) * 3.0;
        center *= gradient_position * VERTICAL_BORDER_DEPTH + (1.0 - VERTICAL_BORDER_DEPTH);
        left *= gradient_position * VERTICAL_BORDER_DEPTH + (1.0 - VERTICAL_BORDER_DEPTH);
        right *= gradient_position * VERTICAL_BORDER_DEPTH + (1.0 - VERTICAL_BORDER_DEPTH);
    }

    /* Blur the edges of the separators of adjacent columns */
    if (sub_pos.x < 1.0 || sub_pos.x > 5.0) {
        pos.y += 0.5;
        pos.y = fract(pos.y);
        
        if (pos.y < 1.0 / 3.0) {
            float gradient_position = pos.y * 3.0;
            if (pos.x < 0.5) {
                gradient_position = 1.0 - (1.0 - gradient_position) * (1.0 - (pos.x) * 6.0);
            }
            else {
                gradient_position = 1.0 - (1.0 - gradient_position) * (1.0 - (1.0 - pos.x) * 6.0);
            }
            center *= gradient_position * VERTICAL_BORDER_DEPTH + (1.0 - VERTICAL_BORDER_DEPTH);
            left *= gradient_position * VERTICAL_BORDER_DEPTH + (1.0 - VERTICAL_BORDER_DEPTH);
            right *= gradient_position * VERTICAL_BORDER_DEPTH + (1.0 - VERTICAL_BORDER_DEPTH);
        }
        else if (pos.y > 2.0 / 3.0) {
            float gradient_position = (1.0 - pos.y) * 3.0;
            if (pos.x < 0.5) {
                gradient_position = 1.0 - (1.0 - gradient_position) * (1.0 - (pos.x) * 6.0);
            }
            else {
                gradient_position = 1.0 - (1.0 - gradient_position) * (1.0 - (1.0 - pos.x) * 6.0);
            }
            center *= gradient_position * VERTICAL_BORDER_DEPTH + (1.0 - VERTICAL_BORDER_DEPTH);
            left *= gradient_position * VERTICAL_BORDER_DEPTH + (1.0 - VERTICAL_BORDER_DEPTH);
            right *= gradient_position * VERTICAL_BORDER_DEPTH + (1.0 - VERTICAL_BORDER_DEPTH);
        }
    }

    
    /* Subpixel blurring, like LCD filter*/
    
    vec4 midleft = mix(left, center, 0.5);
    vec4 midright = mix(right, center, 0.5);
    
    vec4 ret;
    if (sub_pos.x < 1.0) {
        ret = mix(vec4(COLOR_HIGH * center.r, COLOR_LOW * center.g, COLOR_HIGH * left.b, 1),
                  vec4(COLOR_HIGH * center.r, COLOR_LOW * center.g, COLOR_LOW  * left.b, 1),
                  sub_pos.x);
    }
    else if (sub_pos.x < 2.0) {
        ret = mix(vec4(COLOR_HIGH * center.r, COLOR_LOW  * center.g, COLOR_LOW * left.b, 1),
                  vec4(COLOR_HIGH * center.r, COLOR_HIGH * center.g, COLOR_LOW * midleft.b, 1),
                  sub_pos.x - 1.0);
    }
    else if (sub_pos.x < 3.0) {
        ret = mix(vec4(COLOR_HIGH * center.r  , COLOR_HIGH * center.g, COLOR_LOW * midleft.b, 1),
                  vec4(COLOR_LOW  * midright.r, COLOR_HIGH * center.g, COLOR_LOW * center.b, 1),
                  sub_pos.x - 2.0);
    }
    else if (sub_pos.x < 4.0) {
        ret = mix(vec4(COLOR_LOW * midright.r, COLOR_HIGH * center.g , COLOR_LOW  * center.b, 1),
                  vec4(COLOR_LOW * right.r   , COLOR_HIGH  * center.g, COLOR_HIGH * center.b, 1),
                  sub_pos.x - 3.0);
    }
    else if (sub_pos.x < 5.0) {
        ret = mix(vec4(COLOR_LOW * right.r, COLOR_HIGH * center.g  , COLOR_HIGH * center.b, 1),
                  vec4(COLOR_LOW * right.r, COLOR_LOW  * midright.g, COLOR_HIGH * center.b, 1),
                  sub_pos.x - 4.0);
    }
    else {
        ret = mix(vec4(COLOR_LOW  * right.r, COLOR_LOW * midright.g, COLOR_HIGH * center.b, 1),
                  vec4(COLOR_HIGH * right.r, COLOR_LOW * right.g  ,  COLOR_HIGH * center.b, 1),
                  sub_pos.x - 5.0);
    }
    
    return ret;
}

// The curve as SameBoy draws it, over the picture's place in the window's pixels, its edge anti-aliased over a pixel;
// the texture position moved by as many pixels as the curve moves the fragment.
PSOutput main(PSInput input)
{
    float2 position = input.v_uv;
    float edge = 1.0;
    if (image.z > 0.0 && image.w > 0.0) {
        const float2 at = (input.v_position.xy - image.xy) / image.zw;
        float2 curved = at;
        const float y_multiplier = 1.0 / (cos(curved.x - 0.5) * CURVENESS + (1.0 - CURVENESS));
        curved.y = curved.y * y_multiplier - (y_multiplier - 1.0) / 2.0;
        const float x_multiplier = 1.0 / (cos(curved.y - 0.5) * CURVENESS + (1.0 - CURVENESS));
        curved.x = curved.x * x_multiplier - (x_multiplier - 1.0) / 2.0;
        const float2 pixel = curved * image.zw;
        edge = saturate(min(min(pixel.x, image.z - pixel.x), min(pixel.y, image.w - pixel.y)));
        const float2 moved = (curved - at) * image.zw;
        position += moved.x * ddx(input.v_uv) + moved.y * ddy(input.v_uv);
    }
    PSOutput output = filter_main(input, position);
    output.o_color *= edge;
    return output;
}
