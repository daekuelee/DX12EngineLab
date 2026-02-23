#include "common.hlsli"

// Debug line VS: world-space position -> clip space via ViewProj (b0)
struct VSOut
{
    float4 Pos : SV_Position;
};

VSOut VSDebugLine(VSInput input)
{
    VSOut o;
    o.Pos = mul(float4(input.Pos, 1.0f), ViewProj);
    return o;
}
