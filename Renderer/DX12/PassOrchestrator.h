#pragma once

#include <d3d12.h>
#include <cstdint>
#include "RenderContext.h"
#include "GeometryPass.h"

namespace Renderer
{
    class ShaderLibrary;
    class RenderScene;
    class DescriptorRingAllocator;
    class ImGuiLayer;
    struct FrameContext;

    // Input parameters for pass orchestration
    struct PassOrchestratorInputs
    {
        // Command list to record into
        ID3D12GraphicsCommandList* cmd = nullptr;

        // Frame resources
        FrameContext* frame = nullptr;
        DescriptorRingAllocator* descRing = nullptr;
        ShaderLibrary* shaders = nullptr;
        RenderScene* scene = nullptr;
        ImGuiLayer* imguiLayer = nullptr;

        // Render targets
        ID3D12Resource* backBuffer = nullptr;
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = {};
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = {};

        // View state
        D3D12_VIEWPORT viewport = {};
        D3D12_RECT scissor = {};

        // Frame data
        D3D12_GPU_VIRTUAL_ADDRESS frameCBAddress = 0;
        D3D12_GPU_DESCRIPTOR_HANDLE srvTableHandle = {};

        // Geometry pass inputs
        GeometryPassInputs geoInputs = {};
    };

    // Pass enable flags for debugging
    struct PassEnableFlags
    {
        bool clearPass = true;
        bool geometryPass = true;
        bool imguiPass = true;
    };

    // Orchestrates render pass execution in fixed order: Clear -> Geometry -> ImGui
    // Manages backbuffer barrier scope (PRESENT <-> RENDER_TARGET)
    class PassOrchestrator
    {
    public:
        PassOrchestrator() = default;
        ~PassOrchestrator() = default;

        // Execute all enabled passes in order
        // Returns total draw calls recorded
        static uint32_t Execute(const PassOrchestratorInputs& inputs,
                                const PassEnableFlags& flags = PassEnableFlags{});

    private:
        // Build RenderContext from inputs
        static RenderContext BuildRenderContext(const PassOrchestratorInputs& inputs);

        // Setup common render state (root sig, viewport, scissor, RT, topology, heaps, bindings)
        static void SetupRenderState(const PassOrchestratorInputs& inputs);
    };
}
