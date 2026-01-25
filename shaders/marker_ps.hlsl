// marker_ps.hlsl - Diagnostic marker pixel shader

float4 PSMarker() : SV_Target
{
    return float4(1.0, 0.0, 1.0, 1.0); // Magenta for visibility
}
