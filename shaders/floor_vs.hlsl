// floor_vs.hlsl - Floor vertex shader
// Does NOT read transforms - just applies ViewProj directly
// This fixes z-fighting bug where floor was shifted by Transforms[0]

#include "common.hlsli"

struct VSOutput
{
    float4 Pos : SV_Position;
};

VSOutput VSFloor(VSInput vin)
{
    VSOutput o;
    o.Pos = mul(float4(vin.Pos, 1.0), ViewProj);
    return o;
}
