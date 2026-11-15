#!/bin/sh
# Compile launcher/shaders/*.frag.hlsl into the C headers the launcher embeds: SPIR-V (Vulkan), DXIL (Direct3D 12)
# and MSL (Metal) for SDL's GPU renderer.  Run by hand after a shader changes; the headers are committed, so the
# build and the CI need none of these tools.
#
# Either SDL_shadercross's `shadercross` on PATH, or what it wraps: DXC (with libdxil beside it, so the DXIL comes out
# signed) and SPIRV-Cross, named by $DXC and $SPIRV_CROSS or found on PATH.  The options and the steps are shadercross's
# own, the round trip of the DXIL through SPIR-V included.
set -eu

here=$(cd "$(dirname "$0")/.." && pwd)
cd "$here/launcher/shaders"

header() {
    # A blob as `static const unsigned char NAME[]` and its length, named after its file.
    xxd -i "$1" | sed -e 's/^unsigned /static const unsigned /' > "$1.h"
    rm -f "$1"
}

for source in *.frag.hlsl; do
    base=${source%.hlsl}
    echo "$source"
    if command -v shadercross >/dev/null 2>&1 && [ -z "${DXC:-}" ]; then
        shadercross "$source" -o "$base.spv"
        shadercross "$source" -o "$base.dxil"
        shadercross "$source" -o "$base.msl"
    else
        dxc=${DXC:-dxc}
        spirv_cross=${SPIRV_CROSS:-spirv-cross}
        "$dxc" -E main -T ps_6_0 -spirv -fspv-flatten-resource-arrays -fspv-preserve-bindings -fspv-preserve-interface \
            "$source" -Fo "$base.spv"
        # DXIL as shadercross makes it: through SPIR-V and back to HLSL, so that the inputs are named TEXCOORD0 and up by
        # their locations, as SDL's own vertex shaders write them (Direct3D 12 links the stages by name).
        "$spirv_cross" "$base.spv" --hlsl --shader-model 60 --hlsl-enable-compat --hlsl-nonwritable-uav-texture-as-srv \
            --hlsl-flatten-matrix-vertex-input-semantics --output "$base.roundtrip.hlsl"
        "$dxc" -E main -T ps_6_0 "$base.roundtrip.hlsl" -Fo "$base.dxil"
        rm -f "$base.roundtrip.hlsl"
        "$spirv_cross" "$base.spv" --msl --msl-version 10200 --output "$base.msl"
    fi
    header "$base.spv"
    header "$base.dxil"
    header "$base.msl"
done
