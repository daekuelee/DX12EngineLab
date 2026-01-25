// common.hlsli - Shared types and ABI documentation
// Root Signature ABI (must match ShaderLibrary::RootParam enum):
//   RP_FrameCB       = 0 : b0 space0 - Frame constants (ViewProj)
//   RP_TransformsTable = 1 : t0 space0 - Transforms SRV descriptor table
//   RP_InstanceOffset  = 2 : b1 space0 - Instance offset (1 DWORD root constant)
//   RP_DebugCB         = 3 : b2 space0 - Debug constants (ColorMode)

#ifndef COMMON_HLSLI
#define COMMON_HLSLI

// Color mode constants
static const uint COLOR_MODE_FACE_DEBUG = 0;
static const uint COLOR_MODE_INSTANCE_ID = 1;
static const uint COLOR_MODE_LAMBERT = 2;

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

// Debug CB - bound at root slot 3 as 32-bit root constants
cbuffer DebugCB : register(b2, space0)
{
    uint ColorMode;
    uint _pad0, _pad1, _pad2;  // Pad to 16 bytes
};

// Face debug palette
static const float3 kFaceColors[6] = {
    float3(0, 1, 0),      // 0: -Z (front): Green
    float3(1, 1, 0),      // 1: +Z (back): Yellow
    float3(0, 0, 1),      // 2: -X (left): Blue
    float3(1, 0.5, 0),    // 3: +X (right): Orange
    float3(1, 0, 0),      // 4: +Y (top): Red
    float3(0, 1, 1),      // 5: -Y (bottom): Cyan
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
