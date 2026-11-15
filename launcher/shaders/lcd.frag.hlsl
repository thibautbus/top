// The LCD filter of SameBoy (Shaders/LCD.fsh and the gamma of Shaders/MasterShader.fsh, SameBoy 1.0.3), in HLSL for
// SDL's GPU renderer: each of the picture's pixels as red, green and blue sub-pixels, its top and bottom darkened.
// The positions are the drawn texture's own, so the grid follows each part the sub-pixel camera moves, moved by the
// constants' `grid` (in texels) for a part drawn apart.  Over a texture with transparent pixels, which carry the colour
// of the picture around them, the colour is kept to the centre pixel's coverage.
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

cbuffer Context : register(b0, space3) {
    float2 unused;
    float2 grid;
    float4 image;   // the picture's place in the window's pixels (the filters' common constants)
};

Texture2D u_texture : register(t0, space2);
SamplerState u_sampler : register(s0, space2);

struct PSInput {
    float4 v_color : COLOR0;
    float2 v_uv : TEXCOORD0;
};

struct PSOutput {
    float4 o_color : SV_Target;
};

static const float COLOR_LOW = 0.6;
static const float COLOR_HIGH = 1.0;
static const float SCANLINE_DEPTH = 0.2;
static const float GAMMA = 2.2;

float4 texture_relative(float2 texel, float2 size, float2 offset)
{
    const float4 c = u_texture.SampleLevel(u_sampler, (floor(texel) + offset + 0.5) / size, 0.0);
    return float4(pow(c.rgb, GAMMA), c.a);
}

PSOutput main(PSInput input) {
    PSOutput output;
    float2 size;
    u_texture.GetDimensions(size.x, size.y);
    const float2 texel = input.v_uv * size;
    const float2 sub_pos = frac(texel + grid) * 6.0;

    float4 center = texture_relative(texel, size, float2(0, 0));
    float4 left = texture_relative(texel, size, float2(-1, 0));
    float4 right = texture_relative(texel, size, float2(1, 0));
    const float alpha = center.a;

    if (sub_pos.y < 1.0) {
        center = lerp(center, texture_relative(texel, size, float2( 0, -1)), 0.5 - sub_pos.y / 2.0);
        left =   lerp(left,   texture_relative(texel, size, float2(-1, -1)), 0.5 - sub_pos.y / 2.0);
        right =  lerp(right,  texture_relative(texel, size, float2( 1, -1)), 0.5 - sub_pos.y / 2.0);
        center *= sub_pos.y * SCANLINE_DEPTH + (1.0 - SCANLINE_DEPTH);
        left *= sub_pos.y * SCANLINE_DEPTH + (1.0 - SCANLINE_DEPTH);
        right *= sub_pos.y * SCANLINE_DEPTH + (1.0 - SCANLINE_DEPTH);
    }
    else if (sub_pos.y > 5.0) {
        center = lerp(center, texture_relative(texel, size, float2( 0, 1)), (sub_pos.y - 5.0) / 2.0);
        left =   lerp(left,   texture_relative(texel, size, float2(-1, 1)), (sub_pos.y - 5.0) / 2.0);
        right =  lerp(right,  texture_relative(texel, size, float2( 1, 1)), (sub_pos.y - 5.0) / 2.0);
        center *= (6.0 - sub_pos.y) * SCANLINE_DEPTH + (1.0 - SCANLINE_DEPTH);
        left *= (6.0 - sub_pos.y) * SCANLINE_DEPTH + (1.0 - SCANLINE_DEPTH);
        right *= (6.0 - sub_pos.y) * SCANLINE_DEPTH + (1.0 - SCANLINE_DEPTH);
    }

    const float4 midleft = lerp(left, center, 0.5);
    const float4 midright = lerp(right, center, 0.5);

    float4 ret;
    if (sub_pos.x < 1.0) {
        ret = lerp(float4(COLOR_HIGH * center.r, COLOR_LOW * center.g, COLOR_HIGH * left.b, 1),
                   float4(COLOR_HIGH * center.r, COLOR_LOW * center.g, COLOR_LOW  * left.b, 1),
                   sub_pos.x);
    }
    else if (sub_pos.x < 2.0) {
        ret = lerp(float4(COLOR_HIGH * center.r, COLOR_LOW  * center.g, COLOR_LOW * left.b, 1),
                   float4(COLOR_HIGH * center.r, COLOR_HIGH * center.g, COLOR_LOW * midleft.b, 1),
                   sub_pos.x - 1.0);
    }
    else if (sub_pos.x < 3.0) {
        ret = lerp(float4(COLOR_HIGH * center.r  , COLOR_HIGH * center.g, COLOR_LOW * midleft.b, 1),
                   float4(COLOR_LOW  * midright.r, COLOR_HIGH * center.g, COLOR_LOW * center.b, 1),
                   sub_pos.x - 2.0);
    }
    else if (sub_pos.x < 4.0) {
        ret = lerp(float4(COLOR_LOW * midright.r, COLOR_HIGH * center.g , COLOR_LOW  * center.b, 1),
                   float4(COLOR_LOW * right.r   , COLOR_HIGH  * center.g, COLOR_HIGH * center.b, 1),
                   sub_pos.x - 3.0);
    }
    else if (sub_pos.x < 5.0) {
        ret = lerp(float4(COLOR_LOW * right.r, COLOR_HIGH * center.g  , COLOR_HIGH * center.b, 1),
                   float4(COLOR_LOW * right.r, COLOR_LOW  * midright.g, COLOR_HIGH * center.b, 1),
                   sub_pos.x - 4.0);
    }
    else {
        ret = lerp(float4(COLOR_LOW  * right.r, COLOR_LOW * midright.g, COLOR_HIGH * center.b, 1),
                   float4(COLOR_HIGH * right.r, COLOR_LOW * right.g  ,  COLOR_HIGH * center.b, 1),
                   sub_pos.x - 5.0);
    }

    output.o_color = float4(pow(max(ret.rgb, 0.0), 1.0 / GAMMA) * alpha, alpha) * input.v_color;
    return output;
}
