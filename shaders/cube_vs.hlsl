// cube_vs.hlsl - Cube vertex shader (instanced rendering)

#include "common.hlsli"

// Transforms SRV - bound at root slot 1 as descriptor table
StructuredBuffer<TransformData> Transforms : register(t0, space0);

struct VSOutput
{
    float4 Pos : SV_Position;
    float3 WorldPos : TEXCOORD0;
    float3 Normal : TEXCOORD1;    // For Lambert shading
    uint InstanceID : TEXCOORD2;  // For instance coloring
};

VSOutput VSMain(VSInput vin, uint iid : SV_InstanceID)
{
    VSOutput o;
    float4x4 world = Transforms[iid + InstanceOffset].M;
    float3 worldPos = mul(float4(vin.Pos, 1.0), world).xyz;
    o.Pos = mul(float4(worldPos, 1.0), ViewProj);
    o.WorldPos = worldPos;

    // Compute normal from local vertex position (cube normals point along dominant axis)
    float3 absPos = abs(vin.Pos);
    float3 normal = float3(0, 0, 0);
    if (absPos.x >= absPos.y && absPos.x >= absPos.z)
        normal = float3(sign(vin.Pos.x), 0, 0);
    else if (absPos.y >= absPos.x && absPos.y >= absPos.z)
        normal = float3(0, sign(vin.Pos.y), 0);
    else
        normal = float3(0, 0, sign(vin.Pos.z));

    // Transform normal to world space (assumes uniform scale)
    o.Normal = normalize(mul(float4(normal, 0.0), world).xyz);
    o.InstanceID = iid + InstanceOffset;

    return o;
}
