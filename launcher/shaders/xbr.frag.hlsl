// Hyllian's xBR-lv2 filter (mGBA 0.10.5's res/shaders/xbr-lv2.shader, with its defaults: Y weight 48, equality
// threshold 25, lv2 coefficient 2) in HLSL for SDL's GPU renderer.  The scale it anti-aliases its edges for is the one
// the texture is drawn at; the neighbours are read at their texels' centres; as in Hyllian's original, the second
// result mixes from itself and the 30 and 60 degree lines take their own offsets (mGBA's copy takes the first result
// and the 45 degree offset).  Over a texture with transparent pixels, which carry the colour of the picture around
// them, the colour is kept to the centre texel's coverage.
//
// Hyllian's xBR-lv2 Shader
//
// Copyright (C) 2011-2015 Hyllian - sergiogdb@gmail.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// Incorporates some of the ideas from SABR shader. Thanks to Joshua Street.

Texture2D u_texture : register(t0, space2);
SamplerState u_sampler : register(s0, space2);

struct PSInput {
    float4 v_color : COLOR0;
    float2 v_uv : TEXCOORD0;
};

struct PSOutput {
    float4 o_color : SV_Target;
};

static const float XBR_Y_WEIGHT = 48.0;
static const float XBR_EQ_THRESHOLD = 25.0;
static const float XBR_LV2_COEFFICIENT = 2.0;

static const float4 Ao = float4( 1.0, -1.0, -1.0, 1.0 );
static const float4 Bo = float4( 1.0,  1.0, -1.0,-1.0 );
static const float4 Co = float4( 1.5,  0.5, -0.5, 0.5 );
static const float4 Ax = float4( 1.0, -1.0, -1.0, 1.0 );
static const float4 Bx = float4( 0.5,  2.0, -0.5,-2.0 );
static const float4 Cx = float4( 1.0,  1.0, -0.5, 0.0 );
static const float4 Ay = float4( 1.0, -1.0, -1.0, 1.0 );
static const float4 By = float4( 2.0,  0.5, -2.0,-0.5 );
static const float4 Cy = float4( 2.0,  0.0, -1.0, 0.5 );
static const float4 Ci = float4(0.25, 0.25, 0.25, 0.25);

static const float3 Y = float3(0.2126, 0.7152, 0.0722);

static float2 texture_size;
static float2 centre;

float4 df(float4 A, float4 B) { return abs(A - B); }

float c_df(float3 c1, float3 c2)
{
    const float3 d = abs(c1 - c2);
    return d.r + d.g + d.b;
}

bool4 eq(float4 A, float4 B) { return df(A, B) < XBR_EQ_THRESHOLD; }

float4 weighted_distance(float4 a, float4 b, float4 c, float4 d, float4 e, float4 f, float4 g, float4 h)
{
    return df(a, b) + df(a, c) + df(d, e) + df(d, f) + 4.0 * df(g, h);
}

float3 at(float x, float y) { return u_texture.SampleLevel(u_sampler, (centre + float2(x, y)) / texture_size, 0.0).rgb; }

float4 luma(float3 p, float3 q, float3 r, float3 s) { return float4(dot(p, Y), dot(q, Y), dot(r, Y), dot(s, Y)) * XBR_Y_WEIGHT; }

PSOutput main(PSInput input)
{
    u_texture.GetDimensions(texture_size.x, texture_size.y);
    const float2 texel = input.v_uv * texture_size;
    centre = floor(texel) + 0.5;
    const float2 fp = frac(texel);
    const float scale = 1.0 / max(fwidth(texel).x, 1e-5);   // screen pixels a texel

    const float4 delta = float4(1.0, 1.0, 1.0, 1.0) / scale;
    const float4 deltaL = float4(0.5 / scale, 1.0 / scale, 0.5 / scale, 1.0 / scale);
    const float4 deltaU = deltaL.yxwz;

    //    A1 B1 C1
    // A0  A  B  C C4
    // D0  D  E  F F4
    // G0  G  H  I I4
    //    G5 H5 I5
    const float3 A1 = at(-1, -2), B1 = at(0, -2), C1 = at(1, -2);
    const float3 A = at(-1, -1), B = at(0, -1), C = at(1, -1);
    const float3 D = at(-1, 0), E = at(0, 0), F = at(1, 0);
    const float3 G = at(-1, 1), H = at(0, 1), I = at(1, 1);
    const float3 G5 = at(-1, 2), H5 = at(0, 2), I5 = at(1, 2);
    const float3 A0 = at(-2, -1), D0 = at(-2, 0), G0 = at(-2, 1);
    const float3 C4 = at(2, -1), F4 = at(2, 0), I4 = at(2, 1);

    const float4 b = luma(B, D, H, F);
    const float4 c = luma(C, A, G, I);
    const float4 e = luma(E, E, E, E);
    const float4 d = b.yzwx;
    const float4 f = b.wxyz;
    const float4 g = c.zwxy;
    const float4 h = b.zwxy;
    const float4 i = c.wxyz;

    const float4 i4 = luma(I4, C1, A0, G5);
    const float4 i5 = luma(I5, C4, A1, G0);
    const float4 h5 = luma(H5, F4, B1, D0);
    const float4 f4 = h5.yzwx;

    const float4 fx = Ao * fp.y + Bo * fp.x;
    const float4 fx_left = Ax * fp.y + Bx * fp.x;
    const float4 fx_up = Ay * fp.y + By * fp.x;

    const bool4 interp_restriction_lv0 = and(e != f, e != h);
    const bool4 interp_restriction_lv2_left = and(e != g, d != g);
    const bool4 interp_restriction_lv2_up = and(e != c, b != c);

    float4 fx45i = saturate((fx + delta - Co - Ci) / (2.0 * delta));
    float4 fx45 = saturate((fx + delta - Co) / (2.0 * delta));
    float4 fx30 = saturate((fx_left + deltaL - Cx) / (2.0 * deltaL));
    float4 fx60 = saturate((fx_up + deltaU - Cy) / (2.0 * deltaU));

    const float4 wd1 = weighted_distance(e, c, g, i, h5, f4, h, f);
    const float4 wd2 = weighted_distance(h, d, i5, f, i4, b, e, i);

    const bool4 edri = and(wd1 <= wd2, interp_restriction_lv0);
    bool4 edr = and(wd1 < wd2, interp_restriction_lv0);
    bool4 edr_left = and(XBR_LV2_COEFFICIENT * df(f, g) <= df(h, c), interp_restriction_lv2_left);
    bool4 edr_up = and(df(f, g) >= XBR_LV2_COEFFICIENT * df(h, c), interp_restriction_lv2_up);

    edr = and(edr, !and(edri.yzwx, edri.wxyz));
    edr_left = and(and(edr_left, edr), eq(e, c));
    edr_up = and(and(edr_up, edr), eq(e, g));

    fx45 *= float4(edr);
    fx30 *= float4(edr_left);
    fx60 *= float4(edr_up);
    fx45i *= float4(edri);

    const float4 px = float4(df(e, f) <= df(e, h));
    const float4 maximos = max(max(fx30, fx60), max(fx45, fx45i));

    float3 res1 = E;
    res1 = lerp(res1, lerp(H, F, px.x), maximos.x);
    res1 = lerp(res1, lerp(B, D, px.z), maximos.z);

    float3 res2 = E;
    res2 = lerp(res2, lerp(F, B, px.y), maximos.y);
    res2 = lerp(res2, lerp(D, H, px.w), maximos.w);

    const float3 res = lerp(res1, res2, step(c_df(E, res1), c_df(E, res2)));
    const float alpha = u_texture.SampleLevel(u_sampler, centre / texture_size, 0.0).a;
    PSOutput output;
    output.o_color = float4(res * alpha, alpha) * input.v_color;
    return output;
}
