// floor_ps.hlsl - Floor pixel shader

float4 PSFloor() : SV_Target
{
    // Cool gray floor contrasts with warm red/orange cubes
    return float4(0.35, 0.40, 0.45, 1.0);
}
