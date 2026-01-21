#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <cstdint>

namespace Renderer
{
    class Dx12Context
    {
    public:
        static constexpr uint32_t FrameCount = 2;

        Dx12Context() = default;
        ~Dx12Context() = default;

        Dx12Context(const Dx12Context&) = delete;
        Dx12Context& operator=(const Dx12Context&) = delete;

        bool Initialize(HWND hwnd);
        void Render();
        void Shutdown();

    private:
        HWND m_hwnd = nullptr;
        uint32_t m_width = 0;
        uint32_t m_height = 0;

        // Core DX12 objects (forward-declared as COM pointers)
        Microsoft::WRL::ComPtr<IDXGIFactory6> m_factory;
        Microsoft::WRL::ComPtr<IDXGIAdapter1> m_adapter;
        Microsoft::WRL::ComPtr<ID3D12Device> m_device;
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
        Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapChain;

        // RTV heap and backbuffers
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
        uint32_t m_rtvDescriptorSize = 0;
        Microsoft::WRL::ComPtr<ID3D12Resource> m_backBuffers[FrameCount];
        uint32_t m_frameIndex = 0;

        // Command allocator and list (per-frame in a full implementation)
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;

        // Fence for synchronization
        Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
        uint64_t m_fenceValue = 0;
        HANDLE m_fenceEvent = nullptr;

        bool m_initialized = false;
    };
}
