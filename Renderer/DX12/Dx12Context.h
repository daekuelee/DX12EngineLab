#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <cstdint>
#include "FrameContextRing.h"
#include "ShaderLibrary.h"
#include "RenderScene.h"
#include "ResourceRegistry.h"
#include "ResourceStateTracker.h"
#include "DescriptorRingAllocator.h"
#include "GeometryFactory.h"
#include "ImGuiLayer.h"

namespace Renderer
{
    class Dx12Context
    {
    public:
        // Use FrameContextRing's FrameCount (3) for triple buffering
        static constexpr uint32_t FrameCount = FrameContextRing::FrameCount;

        Dx12Context() = default;
        ~Dx12Context() = default;

        Dx12Context(const Dx12Context&) = delete;
        Dx12Context& operator=(const Dx12Context&) = delete;

        bool Initialize(HWND hwnd);
        void Render();
        void Shutdown();

        // Accessors for backbuffer (RTV selection only - NOT for frame resources)
        uint32_t GetBackBufferIndex() const { return m_swapChain ? m_swapChain->GetCurrentBackBufferIndex() : 0; }

    private:
        HWND m_hwnd = nullptr;
        uint32_t m_width = 0;
        uint32_t m_height = 0;

        // Core DX12 objects
        Microsoft::WRL::ComPtr<IDXGIFactory6> m_factory;
        Microsoft::WRL::ComPtr<IDXGIAdapter1> m_adapter;
        Microsoft::WRL::ComPtr<ID3D12Device> m_device;
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
        Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapChain;

        // RTV heap and backbuffers
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
        uint32_t m_rtvDescriptorSize = 0;
        Microsoft::WRL::ComPtr<ID3D12Resource> m_backBuffers[FrameCount];

        // DSV heap and depth buffer
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
        Microsoft::WRL::ComPtr<ID3D12Resource> m_depthBuffer;

        // CBV/SRV/UAV descriptor ring allocator (shader-visible)
        // Reserved slots: 0,1,2 for per-frame transforms SRVs
        // Dynamic ring: for transient allocations
        DescriptorRingAllocator m_descRing;

        // Frame resource management (per-frame allocators, fence-gated reuse)
        FrameContextRing m_frameRing;
        uint64_t m_frameId = 0; // Monotonic counter for frame resource selection

        // Resource registry (handle-based resource ownership)
        ResourceRegistry m_resourceRegistry;

        // Resource state tracker (SOLE authority for state tracking)
        ResourceStateTracker m_stateTracker;

        // Shader library (root sig + PSO)
        ShaderLibrary m_shaderLibrary;

        // Geometry factory (buffer creation with upload sync)
        GeometryFactory m_geometryFactory;

        // Render scene (geometry + transforms)
        RenderScene m_scene;

        // ImGui layer (HUD overlay)
        ImGuiLayer m_imguiLayer;

        // Backbuffer format (stored for ImGui initialization)
        DXGI_FORMAT m_backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

        // Command list (shared, reset per-frame with per-frame allocator)
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;

        // Viewport and scissor
        D3D12_VIEWPORT m_viewport = {};
        D3D12_RECT m_scissorRect = {};

        // Free camera state (public struct for helper function access)
        public:
        struct FreeCamera
        {
            float position[3] = {0.0f, 180.0f, -220.0f};  // Start at preset A
            float yaw = 0.0f;      // Radians, 0 = looking along +Z
            float pitch = -0.5f;   // Radians, negative = looking down
            float fovY = 0.785398163f; // XM_PIDIV4
            float nearZ = 1.0f;
            float farZ = 1000.0f;
            float moveSpeed = 100.0f;   // Units per second
            float lookSpeed = 1.5f;     // Radians per second
        };
        private:
        FreeCamera m_camera;

        // Timer state for delta time calculation
        LARGE_INTEGER m_lastTime = {};
        LARGE_INTEGER m_frequency = {};
        bool m_timerInitialized = false;

        bool m_initialized = false;

        // Initialize() helpers - device and swap chain
        void InitDevice();
        void InitSwapChain();
        void InitRenderTargets();
        void InitDepthBuffer();

        // Initialize() helpers - subsystems
        bool InitFrameResources();
        bool InitShaders();
        bool InitScene();
        bool InitImGui();

        // Camera helpers
        void UpdateCamera(float dt);

        // Phase helpers for Render()
        float UpdateDeltaTime();
        Allocation UpdateFrameConstants(FrameContext& ctx);
        Allocation UpdateTransforms(FrameContext& ctx);
        void RecordBarriersAndCopy(FrameContext& ctx, const Allocation& transformsAlloc);
        void RecordPasses(FrameContext& ctx, const Allocation& frameCBAlloc, uint32_t srvFrameIndex);
        void ExecuteAndPresent(FrameContext& ctx);
    };
}
