// What SameBoy's filters (Shaders/*.fsh, SameBoy 1.0.3) find around them in its Shaders/MasterShader.fsh, for SDL's GPU
// renderer: GLSL's names, the texture read in linear light (gamma 2.2) and a texel away from the one drawn, and the
// colour written back in gamma.  Their own code is kept as SameBoy writes it, in the files that include this one.  The
// positions are the drawn texture's own, so the filters follow each part the sub-pixel camera moves; over a texture
// with transparent pixels, which carry the colour of the picture around them, the colour is kept to the coverage of
// the texel it is read at (where CRT's curve takes it).  The constants' `params.zw` move the grid the masks are drawn
// on (CRT's), in texels, for a part drawn apart; `cell.z` draws them on the picture's own pixels instead, `cell.xy`
// screen pixels each, for a part drawn at another scale (the near view's panels at half the game's).
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
    float4 params;
    float4 image;   // the picture's place in the window's pixels
    float4 cell;    // xy: the screen pixels of one of the picture's pixels, z: the masks drawn on them
};

Texture2D u_texture : register(t0, space2);
SamplerState u_sampler : register(s0, space2);

struct PSInput {
    float4 v_color : COLOR0;
    float2 v_uv : TEXCOORD0;
    float4 v_position : SV_Position;
};

struct PSOutput {
    float4 o_color : SV_Target;
};

#define vec2 float2
#define vec3 float3
#define vec4 float4
#define mix lerp
#define fract frac
#define STATIC
#define sampler2D int
#define GAMMA 2.2

static float2 texture_size;
static float2 grid;   // params.zw
static float2 cells_at;   // the fragment's place in the picture's pixels, where `cell.z` asks for them

// Where the masks stand at a texture position: the texel's own place moved by `grid`, or the picture's pixel there.
float2 grid_position(float2 position, float2 input_resolution)
{
    return cell.z > 0.0 ? cells_at : position * input_resolution + grid;
}

float4 _texture(float2 position)
{
    const float4 c = u_texture.SampleLevel(u_sampler, position, 0.0);
    return float4(pow(c.rgb, GAMMA), c.a);
}

float4 texture_relative(int image_unused, float2 position, float2 offset)
{
    return _texture((floor(position * texture_size) + offset + 0.5) / texture_size);
}

float4 scale(int image, float2 position, float2 input_resolution, float2 output_resolution);

// The filter at the fragment's texture position, its output resolution the texture's at the scale it is drawn at.
PSOutput filter_main(PSInput input, float2 position)
{
    u_texture.GetDimensions(texture_size.x, texture_size.y);
    grid = params.zw;
    const float2 per_pixel = max(fwidth(input.v_uv * texture_size), 1e-5);
    const float4 colour = scale(0, position, texture_size, texture_size / per_pixel);
    const float alpha = u_texture.SampleLevel(u_sampler, (floor(position * texture_size) + 0.5) / texture_size, 0.0).a;
    PSOutput output;
    output.o_color = float4(pow(max(colour.rgb, 0.0), 1.0 / GAMMA) * alpha, alpha) * input.v_color;
    return output;
}
