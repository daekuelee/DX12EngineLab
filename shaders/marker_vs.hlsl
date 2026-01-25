// marker_vs.hlsl - Diagnostic marker vertex shader
// Pass-through (vertices already in clip space)
// Uses empty root signature (no bindings needed)

struct VSInput
{
    float3 Pos : POSITION;
};

struct VSOutput
{
    float4 Pos : SV_Position;
};

VSOutput VSMarker(VSInput vin)
{
    VSOutput o;
    // Pass-through: vertices are already in clip space (NDC)
    o.Pos = float4(vin.Pos, 1.0);
    return o;
}
