#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>
#include "FrameLinearAllocator.h"

namespace Renderer
{
    static constexpr uint32_t InstanceCount = 10000;

    // Per-frame resources that must be fence-gated before reuse
    struct FrameContext
    {
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdAllocator;
        uint64_t fenceValue = 0;

        // Per-frame linear allocator for upload heaps (frame CB + transforms)
        FrameLinearAllocator uploadAllocator;

        // Transforms buffer - default heap (GPU reads as SRV)
        Microsoft::WRL::ComPtr<ID3D12Resource> transformsDefault;

        // Per-frame resource state tracking (fixes #527 barrier mismatch)
        // Initialized to COPY_DEST since that's the creation state
        D3D12_RESOURCE_STATES transformsState = D3D12_RESOURCE_STATE_COPY_DEST;

        // SRV slot index in the shader-visible heap (per-frame to avoid stomp)
        uint32_t srvSlot = 0;
    };

    // Manages per-frame resources with fence-gated reuse
    // Key invariant: frame resources use (frameId % FrameCount), NOT backbuffer index
    class FrameContextRing
    {
    public:
        static constexpr uint32_t FrameCount = 3;

        FrameContextRing() = default;
        ~FrameContextRing() = default;

        // Non-copyable
        FrameContextRing(const FrameContextRing&) = delete;
        FrameContextRing& operator=(const FrameContextRing&) = delete;

        // Initialize ring with device and descriptor heap
        bool Initialize(ID3D12Device* device, ID3D12DescriptorHeap* srvHeap, uint32_t srvHeapIncrementSize);

        // Shutdown and release resources
        void Shutdown();

        // Begin frame: wait for context to be available, return it
        // Uses monotonic frameId internally to select context
        FrameContext& BeginFrame(uint64_t frameId);

        // End frame: signal fence after queue execution
        void EndFrame(ID3D12CommandQueue* queue, FrameContext& ctx);

        // Access fence for external synchronization (e.g., shutdown wait)
        ID3D12Fence* GetFence() const { return m_fence.Get(); }
        uint64_t GetCurrentFenceValue() const { return m_fenceCounter; }

        // Wait for all frames to complete (for shutdown)
        void WaitForAll();

        // Get GPU handle for a frame's SRV slot
        D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpuHandle(uint32_t frameIndex) const;

    private:
        void WaitForFence(uint64_t value);
        bool CreatePerFrameBuffers(ID3D12Device* device, uint32_t frameIndex);
        void CreateSRV(ID3D12Device* device, uint32_t frameIndex);

        FrameContext m_frames[FrameCount];

        Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
        HANDLE m_fenceEvent = nullptr;
        uint64_t m_fenceCounter = 0;

        ID3D12Device* m_device = nullptr; // Non-owning
        ID3D12DescriptorHeap* m_srvHeap = nullptr; // Non-owning
        uint32_t m_srvIncrementSize = 0;
    };
}
