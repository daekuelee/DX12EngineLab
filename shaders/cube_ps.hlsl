// cube_ps.hlsl - Cube pixel shader (multi-mode coloring)

#include "common.hlsli"

struct PSInput
{
    float4 Pos : SV_Position;
    float3 WorldPos : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    uint InstanceID : TEXCOORD2;
    float ScaleY : TEXCOORD3;     // Day3.12: For fixture debug highlighting
};

float3 HueToRGB(float hue)
{
    float3 rgb = abs(hue * 6.0 - float3(3, 2, 4)) * float3(1, -1, -1) + float3(-1, 2, 2);
    return saturate(rgb);
}

float4 PSMain(PSInput pin, uint primID : SV_PrimitiveID) : SV_Target
{
    // MT3: Check for invalid transform marker (magenta output)
    if (pin.InstanceID == 0xFFFFFFFF)
    {
        return float4(1.0, 0.0, 1.0, 1.0);  // Magenta = invalid transform
    }

    float3 color;

    // Day3.12: Direct scaleY visualization as grayscale
    // scaleY=0 -> black, scaleY=1.5 -> white (clamped)
    // This shows exact value shader receives
    float intensity = saturate(pin.ScaleY / 1.5);  // Normalize: 0->0, 1.5->1
    return float4(intensity, intensity, intensity, 1.0);

    if (ColorMode == COLOR_MODE_FACE_DEBUG)
    {
        uint faceIndex = (primID % 12) / 2;  // 12 triangles per cube instance
        color = kFaceColors[faceIndex];
    }
    else if (ColorMode == COLOR_MODE_INSTANCE_ID)
    {
        float hue = frac(pin.InstanceID * 0.618033988749);
        color = HueToRGB(hue);
    }
    else // COLOR_MODE_LAMBERT
    {
        float3 lightDir = normalize(float3(1, 1, -1));
        float NdotL = saturate(dot(normalize(pin.Normal), lightDir));
        color = float3(0.8, 0.8, 0.8) * (0.3 + 0.7 * NdotL);
    }

    // Anti-alias edges using world position derivatives
    float3 worldFrac = frac(pin.WorldPos);
    float3 edgeDist = min(worldFrac, 1.0 - worldFrac);
    float3 fw = fwidth(pin.WorldPos);
    float edgeFactor = smoothstep(0.0, fw.x * 2.0, min(edgeDist.x, edgeDist.z));

    // Darken edges slightly to break up aliasing
    color *= lerp(0.7, 1.0, edgeFactor);
    return float4(color, 1.0);
}
