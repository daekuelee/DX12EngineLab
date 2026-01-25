// cube_ps.hlsl - Cube pixel shader (debug face colors)

struct PSInput
{
    float4 Pos : SV_Position;
    float3 WorldPos : TEXCOORD0;
};

float4 PSMain(PSInput pin, uint primID : SV_PrimitiveID) : SV_Target
{
    // DEBUG MODE: Face-based colors for visual verification
    // Order matches index buffer: -Z, +Z, -X, +X, +Y, -Y
    static const float3 faceDebugColors[6] = {
        float3(0, 1, 0),      // 0: -Z (front): Green
        float3(1, 1, 0),      // 1: +Z (back): Yellow
        float3(0, 0, 1),      // 2: -X (left): Blue
        float3(1, 0.5, 0),    // 3: +X (right): Orange
        float3(1, 0, 0),      // 4: +Y (top): Red
        float3(0, 1, 1),      // 5: -Y (bottom): Cyan
    };

    uint faceIndex = (primID % 12) / 2;  // 12 triangles per cube instance
    return float4(faceDebugColors[faceIndex], 1.0);
}
