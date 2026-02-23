#pragma once

#include <d3d12.h>
#include <cstdint>

namespace Renderer
{
    class ShaderLibrary;
    class DescriptorRingAllocator;

    // CPU-side capsule wireframe vertex generation and GPU draw recording.
    // Generates LINELIST vertex pairs in world space each frame.
    // Vertex layout: float3 (POSITION) per vertex, matching VSInput in common.hlsli.
    class CapsuleWireframe
    {
    public:
        // Maximum vertex count for allocation sizing.
        // 6 rings * 16 segments * 2 verts + 8 vertical lines * 2 verts
        // + 2 hemispheres * 8 arcs * 3 segments * 2 verts = 192 + 16 + 96 = 304
        static constexpr uint32_t MaxVertexCount = 304;
        static constexpr uint32_t VertexStride = sizeof(float) * 3;
        static constexpr uint32_t MaxByteSize = MaxVertexCount * VertexStride;

        // Generate capsule wireframe vertices in world space.
        // dest: CPU-writable buffer of at least MaxByteSize bytes.
        // Returns actual vertex count written.
        static uint32_t GenerateVertices(void* dest,
                                          float posX, float posY, float posZ,
                                          float radius, float halfHeight);

        // Record the draw call using the debug line PSO.
        // Assumes render targets and viewport/scissor are already set.
        // frameCBAddr: GPU VA of FrameCB (ViewProj at b0).
        // vbGpuVA: GPU VA of vertex buffer in upload memory.
        // vertCount: number of vertices returned by GenerateVertices.
        static void RecordDraw(ID3D12GraphicsCommandList* cmd,
                                ShaderLibrary* shaders,
                                D3D12_GPU_VIRTUAL_ADDRESS frameCBAddr,
                                D3D12_GPU_VIRTUAL_ADDRESS vbGpuVA,
                                uint32_t vertCount);
    };
}
