// cube_vs.hlsl - Cube vertex shader (instanced rendering)

#include "common.hlsli"

// Transforms SRV - bound at root slot 1 as descriptor table
StructuredBuffer<TransformData> Transforms : register(t0, space0);

struct VSOutput
{
    float4 Pos : SV_Position;
    float3 WorldPos : TEXCOORD0;
};

VSOutput VSMain(VSInput vin, uint iid : SV_InstanceID)
{
    VSOutput o;
    float4x4 world = Transforms[iid + InstanceOffset].M;
    float3 worldPos = mul(float4(vin.Pos, 1.0), world).xyz;
    o.Pos = mul(float4(worldPos, 1.0), ViewProj);
    o.WorldPos = worldPos;
    return o;
}
