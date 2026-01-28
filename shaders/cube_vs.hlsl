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
    float ScaleY : TEXCOORD3;     // Day3.12: For fixture debug highlighting
};

VSOutput VSMain(VSInput vin, uint iid : SV_InstanceID)
{
    VSOutput o;
    float4x4 world = Transforms[iid + InstanceOffset].M;

    // MT3: Validate transform matrix for NaN/Inf/extreme values
    // Check first and last rows for common invalid patterns
    bool invalid = any(isnan(world[0])) || any(isinf(world[0])) ||
                   any(isnan(world[3])) || any(isinf(world[3])) ||
                   abs(world[3].x) > 10000.0f || abs(world[3].y) > 10000.0f || abs(world[3].z) > 10000.0f;

    if (invalid)
    {
        // Force to origin with magenta flag for visibility
        o.Pos = float4(0, 0, 0.5, 1);  // At screen center
        o.WorldPos = float3(0, 0, 0);
        o.Normal = float3(0, 1, 0);
        o.InstanceID = 0xFFFFFFFF;  // Marker for "invalid" in pixel shader
        o.ScaleY = 999.0;           // High value to avoid fixture highlight
        return o;
    }

    float3 worldPos = mul(float4(vin.Pos, 1.0), world).xyz;
    o.Pos = mul(float4(worldPos, 1.0), ViewProj);
    o.WorldPos = worldPos;

    // Compute normal from local vertex position (cube normals point along dominant axis)
    // Day3.6: Priority Y > X > Z ensures top/bottom faces get correct normals at corners
    float3 absPos = abs(vin.Pos);
    float3 normal = float3(0, 0, 0);
    if (absPos.y >= absPos.x && absPos.y >= absPos.z)
        normal = float3(0, sign(vin.Pos.y), 0);
    else if (absPos.x >= absPos.z)
        normal = float3(sign(vin.Pos.x), 0, 0);
    else
        normal = float3(0, 0, sign(vin.Pos.z));

    // Transform normal to world space (assumes uniform scale)
    o.Normal = normalize(mul(float4(normal, 0.0), world).xyz);
    o.InstanceID = iid + InstanceOffset;

    // Day3.12: Pass scaleY for fixture debug highlighting (row-major: M[1][1] = world._22)
    o.ScaleY = world._22;

    return o;
}
