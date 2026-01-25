#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>

namespace Renderer
{
    struct Allocation {
        void* cpuPtr = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS gpuVA = 0;
        uint64_t offset = 0;  // Offset within upload buffer (for CopyBufferRegion)
    };

    // Per-frame linear allocator for upload heaps.
    // Reset each frame after fence wait - bump pointer allocation, no deallocation.
    class FrameLinearAllocator {
    public:
        bool Initialize(ID3D12Device* device, uint64_t capacity);
        void Reset();  // Called in BeginFrame - resets offset to 0
        Allocation Allocate(uint64_t size, uint64_t alignment = 256);
        void Shutdown();

        uint64_t GetOffset() const { return m_offset; }
        uint64_t GetCapacity() const { return m_capacity; }
        ID3D12Resource* GetBuffer() const { return m_uploadBuffer.Get(); }

    private:
        Microsoft::WRL::ComPtr<ID3D12Resource> m_uploadBuffer;
        uint8_t* m_cpuBasePtr = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS m_gpuBaseVA = 0;
        uint64_t m_offset = 0;
        uint64_t m_capacity = 0;
    };
}
