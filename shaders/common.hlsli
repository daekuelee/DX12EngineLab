// common.hlsli - Shared types and ABI documentation
// Root Signature ABI (must match ShaderLibrary::RootParam enum):
//   RP_FrameCB       = 0 : b0 space0 - Frame constants (ViewProj)
//   RP_TransformsTable = 1 : t0 space0 - Transforms SRV descriptor table
//   RP_InstanceOffset  = 2 : b1 space0 - Instance offset (1 DWORD root constant)

#ifndef COMMON_HLSLI
#define COMMON_HLSLI

// Frame constants - bound at root slot 0 as inline CBV
cbuffer FrameCB : register(b0, space0)
{
    row_major float4x4 ViewProj;
};

// Instance offset - bound at root slot 2 as 32-bit root constant
cbuffer InstanceCB : register(b1, space0)
{
    uint InstanceOffset;
};

// Transform data for instanced rendering
// Bound at root slot 1 as descriptor table (SRV t0)
struct TransformData
{
    row_major float4x4 M;
};

// Common vertex input (position only)
struct VSInput
{
    float3 Pos : POSITION;
};

#endif // COMMON_HLSLI
