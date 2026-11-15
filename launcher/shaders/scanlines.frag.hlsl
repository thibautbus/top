// Scanlines over the game's picture: the bottom of each of the picture's rows darkened, on SDL's GPU renderer.
// The rows are the drawn texture's own, so they follow each part the sub-pixel camera moves, or the picture's own for a
// part drawn at another scale (`cell`).  The sampling is
// SDL's pixel-art sampling (sharp at a whole scale, Fill's smooth edges otherwise).

cbuffer Context : register(b0, space3) {
    float strength;     // how dark the line is: 0 none, 1 black
    float thickness;    // its share of a row, rounded to whole screen pixels, one at least
    float2 grid;        // the rows moved by so many texels, for a part drawn apart to keep the picture's rows
    float4 image;       // the picture's place in the window's pixels (the filters' common constants)
    float4 cell;        // xy: the screen pixels of one of the picture's rows, z: the lines drawn on them
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

// Dark rows from 0 to y, in rows: `band` of each row, at its bottom.
float dark_below(float y, float band)
{
    return floor(y) * band + clamp(frac(y) - (1.0 - band), 0.0, band);
}

PSOutput main(PSInput input) {
    PSOutput output;
    float2 size;
    u_texture.GetDimensions(size.x, size.y);
    const float2 texel = input.v_uv * size;
    const float2 per_pixel = fwidth(texel);   // rows per screen pixel

    const float2 box = clamp(per_pixel, 1e-5, 1.0);
    const float2 tx = texel - 0.5 * box;
    const float2 uv = (floor(tx) + 0.5 + smoothstep(1.0 - box, 1.0, frac(tx))) / size;
    float4 colour = u_texture.SampleGrad(u_sampler, uv, ddx(input.v_uv), ddy(input.v_uv)) * input.v_color;

    // The rows the lines stand on: the texture's, or the picture's own for a part drawn at another scale.
    const float rows = cell.z > 0.0 ? 1.0 / max(cell.y, 1e-5) : per_pixel.y;   // rows per screen pixel
    const float scale = 1.0 / max(rows, 1e-5);   // screen pixels per row
    // A share on half a screen pixel rounds up, whatever the derivative's last bits.
    const float band = max(1.0, floor(scale * thickness + 0.5 + 1e-3)) / scale;
    const float half_pixel = 0.5 * rows;
    const float row = cell.z > 0.0 ? (input.v_position.y - image.y) * rows : texel.y + grid.y;
    const float covered = (dark_below(row + half_pixel, band) - dark_below(row - half_pixel, band)) / rows;
    const float room = saturate(2.0 * (scale - 1.5));   // none under 1.5 pixels a row, whole from 2
    colour.rgb *= 1.0 - strength * room * saturate(covered);

    output.o_color = colour;
    return output;
}
