#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <cstdint>
#include "FrameContextRing.h"
#include "ShaderLibrary.h"
#include "RenderScene.h"

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

        // CBV/SRV/UAV heap (shader-visible) for transforms SRV
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_cbvSrvUavHeap;
        uint32_t m_cbvSrvUavDescriptorSize = 0;

        // Frame resource management (per-frame allocators, fence-gated reuse)
        FrameContextRing m_frameRing;
        uint64_t m_frameId = 0; // Monotonic counter for frame resource selection

        // Shader library (root sig + PSO)
        ShaderLibrary m_shaderLibrary;

        // Render scene (geometry + transforms)
        RenderScene m_scene;

        // Command list (shared, reset per-frame with per-frame allocator)
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;

        // Viewport and scissor
        D3D12_VIEWPORT m_viewport = {};
        D3D12_RECT m_scissorRect = {};

        bool m_initialized = false;

        // Phase helpers for Render()
        float UpdateDeltaTime();
        Allocation UpdateFrameConstants(FrameContext& ctx);
        Allocation UpdateTransforms(FrameContext& ctx);
        void RecordBarriersAndCopy(FrameContext& ctx, const Allocation& transformsAlloc);
        void RecordPasses(FrameContext& ctx, const Allocation& frameCBAlloc, uint32_t srvFrameIndex);
        void ExecuteAndPresent(FrameContext& ctx);
    };
}
