#include "CapsuleWireframe.h"
#include "ShaderLibrary.h"
#include <cmath>

namespace Renderer
{
    // Helper: write a line segment (two float3 vertices) and advance pointer
    static void EmitLine(float*& p,
                         float ax, float ay, float az,
                         float bx, float by, float bz)
    {
        *p++ = ax; *p++ = ay; *p++ = az;
        *p++ = bx; *p++ = by; *p++ = bz;
    }

    static constexpr float kPi = 3.14159265358979323846f;
    static constexpr int kRingSegments = 16;    // Segments per horizontal ring
    static constexpr int kLatitudes = 3;        // Rings per hemisphere (0/30/60 deg)
    static constexpr int kLongitudes = 8;       // Vertical arc lines per hemisphere
    static constexpr int kArcSegments = 3;      // Segments per longitude arc (equator to pole)

    uint32_t CapsuleWireframe::GenerateVertices(void* dest,
                                                 float posX, float posY, float posZ,
                                                 float radius, float halfHeight)
    {
        float* p = static_cast<float*>(dest);

        // Capsule center is at (posX, posY + radius + halfHeight, posZ)
        // Bottom hemisphere center: posY + radius
        // Top hemisphere center:    posY + radius + 2*halfHeight
        const float baseY = posY;  // Feet Y
        const float botCenter = baseY + radius;               // Bottom hemisphere center
        const float topCenter = baseY + radius + 2.0f * halfHeight; // Top hemisphere center

        // Pre-compute ring cos/sin
        float ringCos[kRingSegments];
        float ringSin[kRingSegments];
        for (int i = 0; i < kRingSegments; ++i)
        {
            float angle = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(kRingSegments);
            ringCos[i] = cosf(angle);
            ringSin[i] = sinf(angle);
        }

        // --- Horizontal rings ---
        // Bottom hemisphere: latitudes at 0, 30, 60 degrees below equator
        float latDeg[kLatitudes] = { 0.0f, 30.0f, 60.0f };
        for (int lat = 0; lat < kLatitudes; ++lat)
        {
            float latRad = latDeg[lat] * kPi / 180.0f;
            float ringR = radius * cosf(latRad);
            float dy = -radius * sinf(latRad);  // Below center for bottom hemisphere
            float ringY = botCenter + dy;

            for (int i = 0; i < kRingSegments; ++i)
            {
                int j = (i + 1) % kRingSegments;
                float ax = posX + ringR * ringCos[i];
                float az = posZ + ringR * ringSin[i];
                float bx = posX + ringR * ringCos[j];
                float bz = posZ + ringR * ringSin[j];
                EmitLine(p, ax, ringY, az, bx, ringY, bz);
            }
        }

        // Top hemisphere: latitudes at 0, 30, 60 degrees above equator
        for (int lat = 0; lat < kLatitudes; ++lat)
        {
            float latRad = latDeg[lat] * kPi / 180.0f;
            float ringR = radius * cosf(latRad);
            float dy = radius * sinf(latRad);  // Above center for top hemisphere
            float ringY = topCenter + dy;

            for (int i = 0; i < kRingSegments; ++i)
            {
                int j = (i + 1) % kRingSegments;
                float ax = posX + ringR * ringCos[i];
                float az = posZ + ringR * ringSin[i];
                float bx = posX + ringR * ringCos[j];
                float bz = posZ + ringR * ringSin[j];
                EmitLine(p, ax, ringY, az, bx, ringY, bz);
            }
        }

        // --- Vertical cylinder lines ---
        // 8 lines connecting bottom equator to top equator
        for (int i = 0; i < kLongitudes; ++i)
        {
            float angle = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(kLongitudes);
            float cx = posX + radius * cosf(angle);
            float cz = posZ + radius * sinf(angle);
            EmitLine(p, cx, botCenter, cz, cx, topCenter, cz);
        }

        // --- Hemisphere longitude arcs ---
        // Bottom hemisphere: 8 arcs from equator to south pole
        for (int i = 0; i < kLongitudes; ++i)
        {
            float angle = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(kLongitudes);
            float cA = cosf(angle);
            float sA = sinf(angle);

            for (int s = 0; s < kArcSegments; ++s)
            {
                float lat0 = (kPi / 2.0f) * static_cast<float>(s) / static_cast<float>(kArcSegments);
                float lat1 = (kPi / 2.0f) * static_cast<float>(s + 1) / static_cast<float>(kArcSegments);

                float r0 = radius * cosf(lat0);
                float r1 = radius * cosf(lat1);
                float dy0 = -radius * sinf(lat0);
                float dy1 = -radius * sinf(lat1);

                EmitLine(p,
                         posX + r0 * cA, botCenter + dy0, posZ + r0 * sA,
                         posX + r1 * cA, botCenter + dy1, posZ + r1 * sA);
            }
        }

        // Top hemisphere: 8 arcs from equator to north pole
        for (int i = 0; i < kLongitudes; ++i)
        {
            float angle = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(kLongitudes);
            float cA = cosf(angle);
            float sA = sinf(angle);

            for (int s = 0; s < kArcSegments; ++s)
            {
                float lat0 = (kPi / 2.0f) * static_cast<float>(s) / static_cast<float>(kArcSegments);
                float lat1 = (kPi / 2.0f) * static_cast<float>(s + 1) / static_cast<float>(kArcSegments);

                float r0 = radius * cosf(lat0);
                float r1 = radius * cosf(lat1);
                float dy0 = radius * sinf(lat0);
                float dy1 = radius * sinf(lat1);

                EmitLine(p,
                         posX + r0 * cA, topCenter + dy0, posZ + r0 * sA,
                         posX + r1 * cA, topCenter + dy1, posZ + r1 * sA);
            }
        }

        uint32_t vertCount = static_cast<uint32_t>(p - static_cast<float*>(dest)) / 3;
        return vertCount;
    }

    void CapsuleWireframe::RecordDraw(ID3D12GraphicsCommandList* cmd,
                                       ShaderLibrary* shaders,
                                       D3D12_GPU_VIRTUAL_ADDRESS frameCBAddr,
                                       D3D12_GPU_VIRTUAL_ADDRESS vbGpuVA,
                                       uint32_t vertCount)
    {
        if (!vertCount || !shaders->GetDebugLinePSO())
            return;

        // Set PSO and root signature
        cmd->SetPipelineState(shaders->GetDebugLinePSO());
        cmd->SetGraphicsRootSignature(shaders->GetRootSignature());

        // Bind FrameCB at root slot 0 (b0 - ViewProj)
        cmd->SetGraphicsRootConstantBufferView(RP_FrameCB, frameCBAddr);

        // Set topology to line list
        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

        // Set vertex buffer view from upload GPU VA
        D3D12_VERTEX_BUFFER_VIEW vbv = {};
        vbv.BufferLocation = vbGpuVA;
        vbv.SizeInBytes = vertCount * VertexStride;
        vbv.StrideInBytes = VertexStride;
        cmd->IASetVertexBuffers(0, 1, &vbv);

        // Draw
        cmd->DrawInstanced(vertCount, 1, 0, 0);
    }
}
