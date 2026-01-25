#pragma once

#include <d3d12.h>
#include <cstdint>

namespace Renderer
{
    // Forward declarations
    class ShaderLibrary;
    class RenderScene;
    class DescriptorRingAllocator;
    struct FrameContext;

    // Parameter bundle for render passes
    // Contains per-frame/per-pass parameters to avoid parameter explosion
    // Long-lived globals (device, queues, registries) should NOT be added here
    struct RenderContext
    {
        ID3D12GraphicsCommandList* cmd;

        // Frame resources
        FrameContext* frame;
        DescriptorRingAllocator* descRing;

        // Rendering resources
        ShaderLibrary* shaders;
        RenderScene* scene;

        // Views
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle;
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle;
        D3D12_VIEWPORT viewport;
        D3D12_RECT scissor;

        // Frame data
        D3D12_GPU_VIRTUAL_ADDRESS frameCBAddress;
        D3D12_GPU_DESCRIPTOR_HANDLE srvTableHandle;
    };

} // namespace Renderer
